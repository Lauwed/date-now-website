<script lang="ts">
	import Card from '$lib/components/Card.svelte';

	interface Newsletter {
		name: string;
		url: string;
	}

	interface NewsletterCategory {
		name: string;
		newsletters: Newsletter[];
	}

	const newslettersCategories: NewsletterCategory[] = [
		{
			name: 'Code',
			newsletters: [
				{
					name: 'daily.dev',
					url: 'https://daily.dev/'
				}
			]
		},
		{
			name: 'Accessibility',
			newsletters: [
				{
					name: 'daily.dev',
					url: 'https://daily.dev/'
				}
			]
		}
	];
</script>

<Card>
	<h1>Newsletters I use</h1>

	<p>This is the list of all the newsletters I use to achieve Date.now() issues.</p>
</Card>

{#each newslettersCategories as category}
	<Card>
		<h2>{category.name}</h2>

		<ul>
			{#each category.newsletters as newsletter}
				<li><a href={newsletter.url}>{newsletter.name}</a></li>
			{/each}
		</ul>
	</Card>
{/each}

<style lang="scss">
	p {
		margin: 0;
	}

	ul {
		margin: revert;

		li {
			&::before {
				content: '->';
				display: inline-block;
				margin-right: 16px;
				font-family: 'Jetbrains Mono', monospace;
			}
		}
	}
</style>
