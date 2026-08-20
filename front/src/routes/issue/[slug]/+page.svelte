<script lang="ts">
	import Card from '$lib/components/Card.svelte';
	import Tag from '$lib/components/Tag.svelte';
	import { format, fromUnixTime } from 'date-fns';
	import Article from '$lib/components/Article.svelte';
	import SectionRenderer from '$lib/components/SectionRenderer.svelte';
	import { rgbOf } from '$lib/colors';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();
</script>

{#if !data.success || data.data === null}
	<Card>
		<h1>Error</h1>
		<p>{data.message}</p>
	</Card>
{:else}
	<div class="issue">
		{#if data.isPreview}
			<p class="issue__preview-banner">
				Preview — this issue is <strong>{data.data.status.toLowerCase()}</strong> and is not
				publicly visible. This link expires shortly.
			</p>
		{/if}

		<Card customClass="issue__header">
			<h1 class="issue__title">{data.data.title}</h1>

			<p class="issue__metadata">
				<Tag tag="span">Issue #{data.data.issueNumber}</Tag>
				<span>{format(fromUnixTime(data.data.publishedAt), 'dd/MM/yyyy')}</span>
			</p>

			{#if data.data.authors.length > 0}
				<ul class="issue__authors">
					{#each data.data.authors as author (author.id)}
						<li class="issue__author">
							{#if author.picture?.thumbUrl || author.picture?.url}
								<img
									class="issue__author-avatar"
									src={author.picture.thumbUrl ?? author.picture.url}
									alt={author.picture.alt ?? ''}
									loading="lazy"
								/>
							{/if}

							{#if author.link}
								<a href={author.link} rel="noopener noreferrer" target="_blank">
									{author.username ?? 'Anonymous'}
								</a>
							{:else}
								<span>{author.username ?? 'Anonymous'}</span>
							{/if}
						</li>
					{/each}
				</ul>
			{/if}

			<ul class="issue__tags">
				{#each data.data.tags as tag (tag.name)}
					<Tag tag="li" --color={rgbOf(tag.color)}>{tag.name}</Tag>
				{/each}
			</ul>
		</Card>

		<img
			class="issue__cover"
			alt={data.data.cover?.alt || "Issue's cover"}
			src={data.data.cover?.url || '/article-cover-placeholder.png'}
			width={data.data.cover?.width ?? undefined}
			height={data.data.cover?.height ?? undefined}
		/>

		<Article customClass="issue__content">
			<SectionRenderer sections={data.sections} />
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

		&__preview-banner {
			order: 0;
			width: 100%;
			margin: 0;
			padding: 10px 16px;
			border-radius: $border-radius;
			background: rgba(255, 182, 0, 0.9);
			color: $black;
		}

		flex-direction: column;
		gap: 12px;

		&__title {
			width: 100%;
			margin: 0;
			order: 1;
		}

		&__authors {
			display: flex;
			flex-wrap: wrap;
			align-items: center;
			gap: 12px;
			list-style: none;
			margin: 0;
			padding: 0;
			font-size: 0.875rem;
		}

		&__author {
			display: flex;
			align-items: center;
			gap: 6px;
		}

		&__author-avatar {
			width: 28px;
			height: 28px;
			border-radius: 50%;
			object-fit: cover;
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
