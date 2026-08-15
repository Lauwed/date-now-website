<script lang="ts">
	import ArticlePreview from '$lib/components/ArticlePreview.svelte';
	import ArticlesList from '$lib/components/ArticlesList.svelte';
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
	import VODsButton from '$lib/components/VODsButton.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();
</script>

<Card tag="section" customClass="archive">
	<h1>Passed Issues</h1>

	<p>You can find all the past issues of <strong>Date.now()</strong></p>

	<ul class="archive__links">
		<li>
			<Button variant="primary" commandfor="subscribe-modal" command="show-modal">Subscribe</Button>
		</li>
		<li><VODsButton /></li>
	</ul>
</Card>

<Card tag="section" customClass="archive__latest">
	<h2>Latest Issue</h2>

	{#if data.data.length > 0}
		<ArticlePreview size="large" article={data.data[0]} />
	{:else}
		<p class="archive__latest__empty">No issue yet, sorry 👀😣</p>
	{/if}
</Card>

{#if data.data.length > 1}
	<ArticlesList articles={data.data.slice(1)} title="All past issues" />
{/if}

<style lang="scss">
	.archive {
		&__links {
			display: flex;
			gap: 8px;
		}

		&__latest {
			&__empty {
				opacity: 0.75;
				margin: 0;
			}
		}
	}
</style>
