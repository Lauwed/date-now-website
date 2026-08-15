<script lang="ts">
	import Card from '$lib/components/Card.svelte';
	import Tag from '$lib/components/Tag.svelte';
	import { format, fromUnixTime } from 'date-fns';
	import Article from '$lib/components/Article.svelte';
	import BlockRenderer from '$lib/components/blockRenderer/BlockRenderer.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();
	console.log(data);
</script>

{#if !data.success || data.data === null}
	<Card>
		<h1>Error</h1>
		<p>{data.message}</p>
	</Card>
{:else}
	<div class="issue">
		<Card customClass="issue__header">
			<h1 class="issue__title">{data.data.title}</h1>

			<p class="issue__metadata">
				<Tag tag="span">Issue #{data.data.issueNumber}</Tag>
				<span>{format(fromUnixTime(data.data.publishedAt), 'dd/MM/yyyy')}</span>
			</p>

			<ul class="issue__tags">
				{#each data.data.tags as tag (tag.name)}
					<Tag tag="li" --color={`${tag.color.r}, ${tag.color.g}, ${tag.color.b}`}>{tag.name}</Tag>
				{/each}
			</ul>
		</Card>

		<img
			class="issue__cover"
			alt="Issue's cover"
			src={data.data.coverURL || '/article-cover-placeholder.png'}
		/>

		<Article customClass="issue__content">
			<BlockRenderer blocks={data.data.content} />
		</Article>
	</div>
{/if}

<style lang="scss">
	@use '../../../lib/styles/variables' as *;
	@use '../../../lib/styles/mixins' as *;

	:global(.issue__header) {
		order: 2;
		display: flex;
		flex-wrap: wrap;
		justify-content: space-between;
		align-items: center;
		gap: 12px;
	}

	:global(.issue__content) {
		order: 3;
	}

	.issue {
		display: flex;
		flex-direction: column;
		gap: 12px;

		&__title {
			width: 100%;
			margin: 0;
			order: 1;
		}

		&__metadata {
			display: flex;
			align-items: center;
			gap: 8px;
			margin: 0;
		}

		&__cover {
			order: 1;
			height: 200px;
			object-fit: cover;
			object-position: center;
			border-radius: $border-radius;
			box-shadow: $box-shadow-bold;
			border: 1px solid rgba($white, 0.5);

			@include md {
				height: 300px;
			}

			@include lg {
				height: 400px;
			}
		}
	}
</style>
